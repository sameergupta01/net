/*
 * Copyright (c) 2008 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Development of this code funded by Astaro AG (http://www.astaro.com/)
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include <net/netfilter/nf_tables.h>
#include <net/netfilter/nf_log.h>
#include <linux/netdevice.h>

static const char *nft_log_null_prefix = "";

struct nft_log {
	struct nf_loginfo	loginfo;
	char			*prefix;
	int			family;
};

static void nft_log_eval(const struct nft_expr *expr,
                         struct nft_data data[NFT_REG_MAX + 1],
                         const struct nft_pktinfo *pkt)
{
	const struct nft_log *priv = nft_expr_priv(expr);
	struct net *net = dev_net(pkt->in ? pkt->in : pkt->out);
    
	nf_log_packet(net, priv->family, pkt->hooknum, pkt->skb, pkt->in,
                  pkt->out, &priv->loginfo, "%s", priv->prefix);
}

static const struct nla_policy nft_log_policy[NFTA_LOG_MAX + 1] = {
	[NFTA_LOG_GROUP]	= { .type = NLA_U16 },
	[NFTA_LOG_PREFIX]	= { .type = NLA_STRING },
	[NFTA_LOG_SNAPLEN]	= { .type = NLA_U32 },
	[NFTA_LOG_QTHRESHOLD]	= { .type = NLA_U16 },
};

static int nft_log_init(const struct nft_ctx *ctx,
                        const struct nft_expr *expr,
                        const struct nlattr * const tb[])
{
	struct nft_log *priv = nft_expr_priv(expr);
	struct nf_loginfo *li = &priv->loginfo;
	const struct nlattr *nla;
    
	priv->family = ctx->afi->family;
    
	nla = tb[NFTA_LOG_PREFIX];
	if (nla != NULL) {
		priv->prefix = kmalloc(nla_len(nla) + 1, GFP_KERNEL);
		if (priv->prefix == NULL)
			return -ENOMEM;
		nla_strlcpy(priv->prefix, nla, nla_len(nla) + 1);
	} else
		priv->prefix = (char *)nft_log_null_prefix;
    
	li->type = NF_LOG_TYPE_ULOG;
	if (tb[NFTA_LOG_GROUP] != NULL)
		li->u.ulog.group = ntohs(nla_get_be16(tb[NFTA_LOG_GROUP]));
    
	if (tb[NFTA_LOG_SNAPLEN] != NULL)
		li->u.ulog.copy_len = ntohl(nla_get_be32(tb[NFTA_LOG_SNAPLEN]));
	if (tb[NFTA_LOG_QTHRESHOLD] != NULL) {
		li->u.ulog.qthreshold =
        ntohs(nla_get_be16(tb[NFTA_LOG_QTHRESHOLD]));
	}
    
	return 0;
}

static void nft_log_destroy(const struct nft_expr *expr)
{
	struct nft_log *priv = nft_expr_priv(expr);
    
	if (priv->prefix != nft_log_null_prefix)
		kfree(priv->prefix);
}

static int nft_log_dump(struct sk_buff *skb, const struct nft_expr *expr)
{
	const struct nft_log *priv = nft_expr_priv(expr);
	const struct nf_loginfo *li = &priv->loginfo;
    
	if (priv->prefix != nft_log_null_prefix)
		if (nla_put_string(skb, NFTA_LOG_PREFIX, priv->prefix))
			goto nla_put_failure;
	if (li->u.ulog.group)
		if (nla_put_be16(skb, NFTA_LOG_GROUP, htons(li->u.ulog.group)))
			goto nla_put_failure;
	if (li->u.ulog.copy_len)
		if (nla_put_be32(skb, NFTA_LOG_SNAPLEN,
                         htonl(li->u.ulog.copy_len)))
			goto nla_put_failure;
	if (li->u.ulog.qthreshold)
		if (nla_put_be16(skb, NFTA_LOG_QTHRESHOLD,
                         htons(li->u.ulog.qthreshold)))
			goto nla_put_failure;
	return 0;
    
nla_put_failure:
	return -1;
}

static struct nft_expr_ops nft_log_ops __read_mostly = {
	.name		= "log",
	.size		= NFT_EXPR_SIZE(sizeof(struct nft_log)),
	.owner		= THIS_MODULE,
	.eval		= nft_log_eval,
	.init		= nft_log_init,
	.destroy	= nft_log_destroy,
	.dump		= nft_log_dump,
	.policy		= nft_log_policy,
	.maxattr	= NFTA_LOG_MAX,
};

static int __init nft_log_module_init(void)
{
	return nft_register_expr(&nft_log_ops);
}

static void __exit nft_log_module_exit(void)
{
	nft_unregister_expr(&nft_log_ops);
}

module_init(nft_log_module_init);
module_exit(nft_log_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick McHardy <kaber@trash.net>");
MODULE_ALIAS_NFT_EXPR("log");